/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 *
 * @format
 */

'use strict';

const {
  HERMES_AST_VISITOR_KEYS,
  NODE_CHILD,
  NODE_LIST_CHILD,
} = require('./HermesASTVisitorKeys');

/**
 * The base class for transforming the Hermes AST to the desired output format.
 * Extended by concrete adapters which output an ESTree or Babel AST.
 */
class HermesASTAdapter {
  constructor(options) {
    this.sourceType = 'script';
  }

  /**
   * Transform the input Hermes AST to the desired output format.
   * This modifies the input AST in place instead of constructing a new AST.
   */
  transform(program) {
    // Comments are not traversed via visitor keys
    const comments = program.comments;
    for (let i = 0; i < comments.length; i++) {
      const comment = comments[i];
      this.fixSourceLocation(comment);
      comments[i] = this.mapComment(comment);
    }

    return this.mapNode(program);
  }

  /**
   * Transform a Hermes AST node to the output AST format.
   *
   * This may modify the input node in-place and return that same node, or a completely
   * new node may be constructed and returned. Overriden in child classes.
   */
  mapNode(node) {
    this.fixSourceLocation(node);
    return this.mapNodeDefault(node);
  }

  mapNodeDefault(node) {
    const visitorKeys = HERMES_AST_VISITOR_KEYS[node.type];
    for (const key in visitorKeys) {
      const childType = visitorKeys[key];
      if (childType === NODE_CHILD) {
        const child = node[key];
        if (child != null) {
          node[key] = this.mapNode(child);
        }
      } else if (childType === NODE_LIST_CHILD) {
        const children = node[key];
        for (let i = 0; i < children.length; i++) {
          const child = children[i];
          if (child != null) {
            children[i] = this.mapNode(child);
          }
        }
      }
    }

    return node;
  }

  /**
   * Update the source location for this node depending on the output AST format.
   * This can modify the input node in-place. Overriden in child classes.
   */
  fixSourceLocation(node) {
    throw new Error('Implemented in subclasses');
  }

  setModuleSourceType() {
    this.sourceType = 'module';
  }

  mapComment(node) {
    return node;
  }

  mapEmpty(node) {
    return null;
  }

  mapImportDeclaration(node) {
    if (node.importKind === 'value') {
      this.setModuleSourceType();
    }

    return this.mapNodeDefault(node);
  }

  mapExportDefaultDeclaration(node) {
    this.setModuleSourceType();
    return this.mapNodeDefault(node);
  }

  mapExportNamedDeclaration(node) {
    if (node.exportKind === 'value') {
      this.setModuleSourceType();
    }

    return this.mapNodeDefault(node);
  }

  mapExportAllDeclaration(node) {
    if (node.exportKind === 'value') {
      this.setModuleSourceType();
    }

    return this.mapNodeDefault(node);
  }
}

module.exports = HermesASTAdapter;
