/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 *
 * @format
 */

const HermesASTAdapter = require('./HermesASTAdapter');

class HermesToBabelAdapter extends HermesASTAdapter {
  fixSourceLocation(node) {
    const loc = node.loc;
    if (loc == null) {
      return;
    }

    node.loc = {
      start: loc.start,
      end: loc.end,
    };

    node.start = loc.rangeStart;
    node.end = loc.rangeEnd;
  }

  mapNode(node) {
    this.fixSourceLocation(node);
    switch (node.type) {
      case 'Program':
        return this.mapProgram(node);
      case 'BlockStatement':
        return this.mapNodeWithDirectives(node);
      case 'Empty':
        return this.mapEmpty(node);
      case 'TemplateElement':
        return this.mapTemplateElement(node);
      case 'GenericTypeAnnotation':
        return this.mapGenericTypeAnnotation(node);
      case 'Property':
        return this.mapProperty(node);
      case 'MethodDefinition':
        return this.mapMethodDefinition(node);
      case 'ImportDeclaration':
        return this.mapImportDeclaration(node);
      case 'ExportDefaultDeclaration':
        return this.mapExportDefaultDeclaration(node);
      case 'ExportNamedDeclaration':
        return this.mapExportNamedDeclaration(node);
      case 'ExportAllDeclaration':
        return this.mapExportAllDeclaration(node);
      default:
        return this.mapNodeDefault(node);
    }
  }

  mapProgram(node) {
    // Visit child nodes and convert to directives
    const {comments, ...program} = this.mapNodeWithDirectives(node);

    program.sourceType = this.sourceType;

    // Adjust start loc to beginning of file
    program.loc.start = {line: 1, column: 0};
    program.start = 0;

    // Adjust end loc to include last comment if program ends with a comment
    if (comments.length > 0) {
      const lastComment = comments[comments.length - 1];
      if (lastComment.end > program.end) {
        program.loc.end = lastComment.loc.end;
        program.end = lastComment.end;
      }
    }

    // Rename root node to File node and move Program node under program property
    return {
      type: 'File',
      loc: program.loc,
      start: program.start,
      end: program.end,
      program,
      comments,
    };
  }

  mapNodeWithDirectives(node) {
    const directives = [];
    for (const child of node.body) {
      if (child.type === 'ExpressionStatement' && child.directive != null) {
        // Visit directive children
        const directiveChild = this.mapNode(child);

        // Modify string literal node to be DirectiveLiteral node
        directiveChild.expression.type = 'DirectiveLiteral';

        // Construct Directive node with DirectiveLiteral value
        directives.push({
          type: 'Directive',
          loc: directiveChild.loc,
          start: directiveChild.start,
          end: directiveChild.end,
          value: directiveChild.expression,
        });
      } else {
        // Once we have found the first non-directive node we know there cannot be any more directives
        break;
      }
    }

    // Move directives from body to new directives array
    node.directives = directives;
    if (directives.length !== 0) {
      node.body = node.body.slice(directives.length);
    }

    // Visit expression statement children
    const body = node.body;
    for (let i = 0; i < body.length; i++) {
      const child = body[i];
      if (child != null) {
        body[i] = this.mapNode(child);
      }
    }

    return node;
  }

  mapTemplateElement(node) {
    // Adjust start loc to exclude "`" at beginning of template literal if this is the first quasi,
    // otherwise exclude "}" from previous expression.
    const startCharsToExclude = 1;

    // Adjust end loc to exclude "`" at end of template literal if this is the last quasi,
    // otherwise exclude "${" from next expression.
    const endCharsToExclude = node.tail ? 1 : 2;

    return {
      type: 'TemplateElement',
      loc: {
        start: {
          line: node.loc.start.line,
          column: node.loc.start.column + startCharsToExclude,
        },
        end: {
          line: node.loc.end.line,
          column: node.loc.end.column - endCharsToExclude,
        },
      },
      start: node.start + startCharsToExclude,
      end: node.end - endCharsToExclude,
      tail: node.tail,
      value: {
        cooked: node.cooked,
        raw: node.raw,
      },
    };
  }

  mapGenericTypeAnnotation(node) {
    // Convert simple `this` generic type to ThisTypeAnnotation
    if (
      node.typeParameters === null &&
      node.id.type === 'Identifier' &&
      node.id.name === 'this'
    ) {
      return {
        type: 'ThisTypeAnnotation',
        loc: node.loc,
        start: node.start,
        end: node.end,
      };
    }

    return this.mapNodeDefault(node);
  }

  mapProperty(node) {
    const key = this.mapNode(node.key);
    const value = this.mapNode(node.value);

    // Convert methods, getters, and setters to ObjectMethod nodes
    if (node.method || node.kind !== 'init') {
      // Properties under the FunctionExpression value that should be moved
      // to the ObjectMethod node itself.
      const {
        id,
        params,
        body,
        async,
        generator,
        returnType,
        typeParameters,
        predicate,
      } = value;

      return {
        type: 'ObjectMethod',
        loc: node.loc,
        start: node.start,
        end: node.end,
        // Non getter or setter methods have `kind = method`
        kind: node.kind === 'init' ? 'method' : node.kind,
        computed: node.computed,
        key,
        id,
        params,
        body,
        async,
        generator,
        returnType,
        typeParameters,
        predicate,
      };
    } else {
      // Non-method property nodes should be renamed to ObjectProperty
      node.type = 'ObjectProperty';
      return node;
    }
  }

  mapMethodDefinition(node) {
    const key = this.mapNode(node.key);
    const value = this.mapNode(node.value);

    // Properties under the FunctionExpression value that should be moved
    // to the ClassMethod node itself.
    const {
      id,
      params,
      body,
      async,
      generator,
      returnType,
      typeParameters,
      predicate,
    } = value;

    return {
      type: 'ClassMethod',
      loc: node.loc,
      start: node.start,
      end: node.end,
      kind: node.kind,
      computed: node.computed,
      static: node.static,
      key,
      id,
      params,
      body,
      async,
      generator,
      returnType,
      typeParameters,
      predicate,
    };
  }
}

module.exports = HermesToBabelAdapter;
